import java.io.IOException;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;
import org.apache.hadoop.mapreduce.lib.input.NLineInputFormat;
import java.io.BufferedReader;
import org.apache.hadoop.mapreduce.RecordReader;
import org.apache.hadoop.io.LongWritable;
import java.io.IOException;
import java.io.InputStreamReader;
import org.apache.hadoop.mapreduce.InputSplit;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.hadoop.mapreduce.lib.input.FileSplit;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileSystem;
import java.util.*;

public class SleepTime {
    public static class TokenizerMapper extends Mapper<Object, Text, Text, IntWritable>{
  
      private final static IntWritable one = new IntWritable(1);
      private Text word = new Text(); // this is the word to which assign the values
      Boolean firstLine = true;
      Map<String, String> fileLines = new HashMap<String, String>();
  
      public void map(Object key, Text value, Context context) throws IOException, InterruptedException {
        String line  = value.toString();
        if(line.startsWith("T")){ // get the line with the time
          fileLines.put("time", line);
        }
        else if (line.startsWith("U")){ // line with the link
          fileLines.put("link", line);
        }
        else if (line.startsWith("W")){ // line with the tweet
          fileLines.put("tweet", line);
        }
  
        if(fileLines.size() == 3){ // there are 3 key which makes a record
          // get the time
          String time = fileLines.get("time");
          // get the hour from the time
          StringTokenizer tkn = new StringTokenizer(time);
          int index = 0;
          String hour = new String();
          while(tkn.hasMoreTokens()){
            hour = tkn.nextToken();
            if(index++ == 2){ // get only the hours
              hour = hour.substring(0,2);
              break;
            }
          }
          // check if the tweet has the sleep word
          String tweet = fileLines.get("tweet").toLowerCase();
          if(tweet.contains("sleep")){ // check if the tweet contains sleep in the word
            word.set(hour);
            context.write(word, one);
          }
          fileLines.clear();
        }
      }
    }
  
    public static class IntSumReducer extends Reducer<Text,IntWritable,Text,IntWritable> {
      private IntWritable result = new IntWritable();
  
      public void reduce(Text key, Iterable<IntWritable> values,Context context) throws IOException, InterruptedException {
        int sum = 0;
        for (IntWritable val : values) {
          sum += val.get();
        }
        result.set(sum);
        context.write(key, result);
      }
    }
  
    public static void main(String[] args) throws Exception {
      Configuration conf = new Configuration();
      Job job = Job.getInstance(conf, "sleep time analysis");
      job.setJarByClass(SleepTime.class);
      job.setMapperClass(TokenizerMapper.class);
      job.setCombinerClass(IntSumReducer.class);
      job.setReducerClass(IntSumReducer.class);
      job.setOutputKeyClass(Text.class);
      job.setOutputValueClass(IntWritable.class);
      FileInputFormat.addInputPath(job, new Path(args[0]));
      FileOutputFormat.setOutputPath(job, new Path(args[1]));
      System.exit(job.waitForCompletion(true) ? 0 : 1);
    }
  }
